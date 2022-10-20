#pragma once

#include "base.hpp"

#include <cstddef>
#include <locale>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"

namespace scopi
{

    class contact_brute_force;

    /**
     * @brief Parameters for contact_brute_force.
     *
     * Specialization of ContactsParams.
     */
    template<>
    struct ContactsParams<contact_brute_force>
    {
        /**
         * @brief Default constructor.
         */
        ContactsParams();

        void init_options(CLI::App& app);

        /**
         * @brief Maximum distance between two neighboring particles.
         *
         * Default value: 2.
         * \note \c dmax > 0
         */
        double dmax;
    };

    /**
     * @brief Brute force contatcs.
     *
     * Contacts between particles are computed using brute force algorithm.
     */
    class contact_brute_force: public contact_base<contact_brute_force>
    {
    public:
        /**
         * @brief Alias for the base class contact_base.
         */
        using base_type = contact_base<contact_brute_force>;

        /**
         * @brief Constructor.
         *
         * @param params [in] Parameters.
         */
        contact_brute_force(const ContactsParams<contact_brute_force>& params = ContactsParams<contact_brute_force>());

        /**
         * @brief Compute contacts between particles using brute force algorithm.
         *
         * Only the contact between particles \c i and \c j is computed, not the contact between \c j and \c i, with \c i < \c j.
         *
         * The returned array of neighbors is sorted.
         * See sort_contacts.
         *
         * @tparam dim Dimension (2 or 3).
         * @param particles [in] Array of particles.
         * @param active_ptr [in] Index of the first active particle.
         *
         * @return Array of neighbors.
         */
        template <std::size_t dim>
        std::vector<neighbor<dim>> run_impl(scopi_container<dim>& particles, std::size_t active_ptr);

    };

    template <std::size_t dim>
    std::vector<neighbor<dim>> contact_brute_force::run_impl(scopi_container<dim>& particles, std::size_t active_ptr)
    {
        std::vector<neighbor<dim>> contacts;
        tic();
        #pragma omp parallel for
        for (std::size_t i = active_ptr; i < particles.pos().size() - 1; ++i)
        {
            for (std::size_t j = i + 1; j < particles.pos().size(); ++j)
            {
                compute_exact_distance(particles, i, j, contacts, this->m_params.dmax);
            }
        }

        // obstacles
        for (std::size_t i = 0; i < active_ptr; ++i)
        {
            for (std::size_t j = active_ptr; j < particles.pos().size(); ++j)
            {
                compute_exact_distance(particles, i, j, contacts, this->m_params.dmax);
            }
        }

        auto duration = toc();
        PLOG_INFO << "----> CPUTIME : compute " << contacts.size() << " contacts = " << duration;

        tic();
        sort_contacts(contacts);
        duration = toc();
        PLOG_INFO << "----> CPUTIME : sort " << contacts.size() << " contacts = " << duration;

        return contacts;

    }
}
